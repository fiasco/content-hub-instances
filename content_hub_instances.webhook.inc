<?php

use \GuzzleHttp\Exception\ServerException;
use \GuzzleHttp\Exception\ConnectException;
use \GuzzleHttp\Exception\ClientException;
use \GuzzleHttp\Exception\RequestException;

/**
 * Webhook callback.
 */
 function content_hub_instances_webhook($client) {
   // If we're not familiar with a client (i.e. it didn't load)
   // and we're recieving a webhook request, then its likely
   // the client was previously registered but has since been
   // lost with a database restore or something.
   // Plexus doesn't know this, and if we give it a 404, it'll
   // just keep trying for 4 days (DOC-11312). So instead, lets
   // let Plexus this that the webhook is still around and that the
   // payload was recieved successfully.
   if (empty($client)) {
     return;
   }
   content_hub_instances_masquerade_credentials($client);
   module_load_include('inc', 'content_hub_connector', 'content_hub_connector.webhook');
   content_hub_connector_webhook();
 }

/**
 * Helper function to automatically register the webhook endpoint.
 *
 * @param object $client
 *   The client of the local webhook endpoint to be registered.
 *
 * @return bool
 *   Whether the operation succeeded or not.
 */
function content_hub_instances_register_webhook($client) {
  // Instantiate the client.
  $api = content_hub_instances_client_connection($client);

  // Check if the webhook already exists.
  if ($webhook = $api->getSettings()->getWebhook($client->webhook_url)) {

    // If the webhook exists and its the same UUID, then here is nothing
    // more for us to do here.
    if ($webhook['uuid'] == $client->webhook_uuid) {
      return TRUE;
    }

    // Its different so lets drop it.
    $api->deleteWebhook($client->webhook_url);
  }

  $success = FALSE;
  try {
    $webhook = $api->addWebhook($client->webhook_url);
    $client->webhook_uuid = $webhook['uuid'];
    $client->webhook_url = $webhook['url'];

    drupal_set_message(t('Webhooks have been enabled. This site will now receive updates from Content Hub.'), 'status');
    $success = TRUE;
    watchdog('content_hub_instances', 'Successful registration of Webhook URL = @URL', array(
      '@URL' => $webhook['url'],
    ));
  }
  catch (ServerException $ex) {
    $msg = $ex->getMessage();
    drupal_set_message(t('Could not reach the Content Hub. Please verify your hostname and Credentials. [Error message: @msg]', array(
      '@msg' => $msg,
    )), 'error');
    watchdog('content_hub_instances', 'Could not reach the Content Hub [Error message: @msg]', array(
      '@msg' => $msg,
    ), WATCHDOG_ERROR);
  }
  catch (ConnectException $ex) {
    $msg = $ex->getMessage();
    drupal_set_message(t('Could not reach the Content Hub. Please verify your hostname URL. [Error message: @msg]', array(
      '@msg' => $msg,
    )), 'error');
    watchdog('content_hub_instances', 'Could not reach the Content Hub. Please verify your hostname URL. [Error message: @msg]', array(
      '@msg' => $msg,
    ), WATCHDOG_ERROR);
  }
  catch (ClientException $ex) {
    $response = $ex->getResponse()->json();
    if (isset($response) && isset($response['error']) && $error = $response['error']) {
      drupal_set_message(t('There was a problem trying to register Webhook URL = %URL. Please try again. (Error Code = @error_code: @error_message)', array(
        '%URL' => $client->webhook_url,
        '@error_code' => $error['code'],
        '@error_message' => $error['message'],
      )), 'error');
      watchdog('content_hub_instances', 'There was a problem trying to register Webhook URL = %URL. (Error Code = @error_code: @error_message)', array(
        '%URL' => $client->webhook_url,
        '@error_code' => $error['code'],
        '@error_message' => $error['message'],
      ), WATCHDOG_ERROR);
    }
  }
  catch (RequestException $ex) {
    $msg = $ex->getMessage();
    drupal_set_message(t('Error trying to connect to the Content Hub (Error Message = @error_message)', array(
      '@error_message' => $msg,
    )), 'error');
    watchdog('content_hub_instances', 'Error trying to connect to the Content Hub (Error Message = @error_message)', array(
      '@error_message' => $msg,
    ), WATCHDOG_ERROR);
  }

  return $success;
}
