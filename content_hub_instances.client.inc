<?php

use Acquia\ContentHubClient;
use \GuzzleHttp\Exception\ConnectException;
use \GuzzleHttp\Exception\ClientException;
use \GuzzleHttp\Exception\RequestException;

/**
 * API credentials client registration form.
 */
 function content_hub_instances_client_form($form, $form_state, $entity) {
   module_load_include('inc', 'content_hub_connector', 'content_hub_connector.admin');
   $client_name = _content_hub_connector_suggest_client_name();

   $form['instance'] = array(
     '#type' => 'hidden',
     '#value' => $entity->uuid,
     '#required' => TRUE,
     '#description' => t('The API endpoint this client is registered against.'),
   );
   $form['name'] = array(
    '#type' => 'item',
    '#title' => 'API Endpoint',
    '#markup' => $entity->name,
   );

   $form['client_name'] = array(
     '#type' => 'textfield',
     '#title' => t('Client Name'),
     '#default_value' => $client_name,
     '#required' => TRUE,
     '#description' => t('A unique client name by which the Content Hub will identify this site. The name of this client site cannot be changed once set.'),
   );

   $form['register_webhook'] = array(
     '#type' => 'checkbox',
     '#title' => 'Register Webhook',
     '#description' => t('This will allow Content Hub to be able to contact this site. This is used for content notifications and to retrieve content from your site.'),
   );

   $webhook_url = url('', array(
      'absolute' => TRUE,
   ));
   $form['webhook_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Webhook URL'),
    '#default_value' => $webhook_url,
    '#field_suffix' => htmlentities('content-hub/webhook/<origin-uuid>'),
    '#description' => t('Please use a full URL (Ex. <i>http://example.com/webhook</i>). This is the end-point where this site will receive webhooks from Content Hub.'),
    '#states' => array(
      // Hide the settings when the cancel notify checkbox is disabled.
      'invisible' => array(
       ':input[name="register_webhook"]' => array('checked' => FALSE),
      ),
    ),
  );

   $form['submit'] = array(
     '#type' => 'submit',
     '#value' => t('Register'),
   );

   return $form;
 }

function content_hub_instances_client_form_validate($form, $form_state) {
  $entity = content_hub_instance_load($form_state['values']['instance']);

  $client = new ContentHubClient\ContentHub($entity->api_key, $entity->secret_key, '', ['base_url' => $entity->hostname]);

  try {
    $site = $client->getClientByName($form_state['values']['client_name']);
    // Client name is not editable through the interface, so only trigger this
    // error message if the origin has not been set already.
    if (isset($site['uuid'])) {
      form_set_error('client_name', t('The client name "%name" is already being used. Insert another one.', array(
        '%name' => $name,
      )));
    }
  }
  catch (ConnectException $ex) {
    $msg = $ex->getMessage();
    form_set_error('hostname', t('Could not reach the Content Hub. Please verify your hostname URL. [Error message: @msg]', array(
      '@msg' => $msg,
    )));
  }
  catch (ClientException $ex) {
    // If we get status code = 404, then this site name is available.
    $code = $ex->getResponse()->getStatusCode();
    $msg = $ex->getMessage();
    if ($code == 404) {
      // All good! Site name is available!
    }
    else {
      form_set_error('client_name', t('Content Hub returned the following error (code = @code): @msg', array(
        '@code' => $code,
        '@msg' => $msg,
      )));
    }
  }
  catch (RequestException $ex) {
    // Some error connecting to Content Hub... are your credentials set
    // correctly?
    $msg = $ex->getMessage();
    form_set_error('secret_key', t("Couldn't get authorization from Content Hub. Are your credentials inserted correctly? The following error was returned: @msg", array(
      '@msg' => $msg,
    )));
  }
}

 function content_hub_instances_client_form_submit($form, $form_state) {
   $client = (object) $form_state['values'];
   if (content_hub_instances_register_client($client)) {
     // We have to save the client before a webhook can be registered.
     $client->webhook_url .= 'content-hub/webhook/' . $client->origin;
     content_hub_instances_client_save($client);

     // Webhook registration.
     if (!empty($form_state['values']['register_webhook'])) {
       require_once dirname(__FILE__) . '/content_hub_instances.webhook.inc';
       content_hub_instances_register_webhook($client) &&
       // Now with the webhook registered, update the client with the webhook
       // information.
       content_hub_instances_client_save($client);
     }

   }
   $form_state['redirect'] = array('admin/config/services/content-hub');
 }

/**
 * Helper function to automatically register the site as client.
 *
 * @param object $client
 *   The configuration client to register with.
 *
 * @return string
 *   Status flag indicating the outcome of the operation.
 */
function content_hub_instances_register_client($client) {
  $entity = content_hub_instance_load($client->instance);
  // $encrypted_secret = $entity->secret_key;
  // $decrypted_secret = $entity->secret_key;

  // Only reset the secret if it is passed. If encryption is activated,
  // then encrypt it too.
  // $encryption = variable_get('content_hub_connector_encryption_key_file', '');
  //
  // // Encrypting the secret, to save for later use.
  // if (!empty($encryption)) {
  //   $encrypted_secret = content_hub_connector_cipher()->encrypt($entity->secret_key);
  //   $decrypted_secret = $entity->secret_key;
  // }

  $api = new ContentHubClient\ContentHub($entity->api_key, $entity->secret_key, '', ['base_url' => $entity->hostname]);

  // Register Client.
  $success = FALSE;
  try {
    $site = $api->register($client->client_name);

    // Registration successful. Setting up the origin and other variables.
    $client->origin = $site['uuid'];

    drupal_set_message(t('Successful Client registration with name "@name" (UUID = @uuid)', array(
      '@name' => $client->client_name,
      '@uuid' => $client->origin,
    )), 'status');
    watchdog('content_hub_instances', 'Successful Client registration with name "@name" (UUID = @uuid)', array(
      '@name' => $client->client_name,
      '@uuid' => $client->origin,
    ));
    $success = TRUE;
  }
  catch (ClientException $ex) {
    $response = $ex->getResponse()->json();
    if (isset($response) && isset($response['error']) && $error = $response['error']) {
      drupal_set_message(t('Error registering client with name="@name" (Error Code = @error_code: @error_message)', array(
        '@error_code' => $error['code'],
        '@name' => $client->client_name,
        '@error_message' => $error['message'],
      )), 'error');
      watchdog('content_hub_instances', 'Error registering client with name="@name" (Error Code = @error_code: @error_message)', array(
        '@error_code' => $error['code'],
        '@name' => $client->client_name,
        '@error_message' => $error['message'],
      ), WATCHDOG_ERROR);
    }
  }
  catch (RequestException $ex) {
    // Some error connecting to Content Hub... are your credentials set
    // correctly?
    $msg = $ex->getMessage();
    form_set_error('secret_key', t("Couldn't get authorization from Content Hub. Are your credentials inserted correctly? The following error was returned: @msg", array(
      '@msg' => $msg,
    )));
  }
  return $success;
}

/**
 * Deregister client.
 */
 function content_hub_instances_client_deregister_form($form, $form_state, $client) {
   $form['origin'] = array(
     '#type' => 'hidden',
     '#value' => $client->origin,
   );
   return confirm_form($form, 'Are you sure you want to deregister this client?', 'admin/config/services/content-hub');
 }

 function content_hub_instances_client_deregister_form_submit($form, $form_state) {
   $client = content_hub_instances_client_load($form_state['values']['origin']);
   content_hub_instances_client_delete($client);
 }

 function content_hub_instances_client_promote_form($form, $form_state, $client) {
   $form['origin'] = array(
     '#type' => 'hidden',
     '#value' => $client->origin,
   );
   return confirm_form($form, 'Are you sure you want to promote this client?', 'admin/config/services/content-hub');
 }

 function content_hub_instances_client_promote_form_submit($form, $form_state) {
   $client = content_hub_instances_client_load($form_state['values']['origin']);
   content_hub_instances_client_promote($client);
 }
