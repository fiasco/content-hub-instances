<?php

use \GuzzleHttp\Exception\ServerException;
use \GuzzleHttp\Exception\ConnectException;
use \GuzzleHttp\Exception\ClientException;
use \GuzzleHttp\Exception\RequestException;

/**
 * Webhook callback.
 */
 function content_hub_instances_webhook($client) {
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
