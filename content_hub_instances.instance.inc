<?php

use Acquia\ContentHubClient;
use \GuzzleHttp\Exception\ConnectException;
use \GuzzleHttp\Exception\ClientException;
use \GuzzleHttp\Exception\RequestException;

/**
 * List Content Hub Connections.
 */
function content_hub_instances_list() {
  $list = [];
  ctools_include('export');
  foreach (ctools_export_crud_load_all('content_hub_instance') as $entity) {
    $list[$entity->uuid] = [
      'name' => $entity->name,
      'hostname' => $entity->hostname,
      'api_key' => $entity->api_key,
      'type' => $entity->type,
    ];
    $operations = [];
    $operations['edit'] = array(
      'title' => t('edit'),
      'href' => 'admin/config/services/content-hub/' . $entity->uuid . '/edit',
      'query' => ['destination' => 'admin/config/services/content-hub'],
    );

    $operations['register'] = array(
      'title' => t('Register client'),
      'href' => 'admin/config/services/content-hub/' . $entity->uuid . '/register',
      'query' => ['destination' => 'admin/config/services/content-hub'],
    );

    $operations['delete'] = array(
      'title' => t('delete'),
      //'href' => 'admin/config/services/content-hub/' . $entity->origin . '/delete',
      'query' => ['destination' => 'admin/config/services/content-hub'],
    );

    $list[$entity->uuid]['operations'] = array(
      'data' => array(
        '#theme' => 'links',
        '#links' => $operations,
        '#attributes' => array('class' => array('links', 'inline')),
      ),
    );
  }
  $form['entities_wrapper'] = array(
    '#type' => 'fieldset',
    '#title' => 'API Credentials',
  );
  $form['entities_wrapper']['entities'] = array(
    '#theme' => 'table',
    '#header' => ['Name', 'Hostname', 'API Key', 'Status', 'Operations'],
    '#rows' => $list,
    '#empty' => t('No content available.'),
  );

  $list = [];
  foreach (ctools_export_crud_load_all('content_hub_client') as $client) {
    $entity = content_hub_instance_load($client->instance);
    $list[$client->origin] = [
      'name' => $entity->name,
      'client_name' => $client->client_name,
      'origin' => $client->origin,
      'webhook' => empty($client->webhook_uuid) ? 'Disabled' : 'Enabled',
      'type' => $client->type,
    ];
    $operations = [];
    $operations['edit'] = array(
      'title' => t('deregister'),
      'href' => 'admin/config/services/content-hub/client/' . $client->origin . '/deregister',
      'query' => ['destination' => 'admin/config/services/content-hub'],
    );

    if (variable_get('content_hub_connector_origin', '') != $client->origin) {
      $operations['promote'] = array(
        'title' => t('Promote'),
        'href' => 'admin/config/services/content-hub/client/' . $client->origin . '/promote',
        'query' => ['destination' => 'admin/config/services/content-hub'],
      );
    }
    else {
      $operations['promote'] = array(
        'title' => t('Promoted'),
      );
    }
    $list[$client->origin]['operations'] = array(
      'data' => array(
        '#theme' => 'links',
        '#links' => $operations,
        '#attributes' => array('class' => array('links', 'inline')),
      ),
    );
  }
  $form['clients_wrapper'] = array(
    '#type' => 'fieldset',
    '#title' => 'Registered Clients',
  );
  $form['clients_wrapper']['clients'] = array(
    '#theme' => 'table',
    '#header' => ['API', 'Client name', 'Origin UUID', 'Webhook', 'Status', 'Operations'],
    '#rows' => $list,
    '#empty' => t('No content available.'),
  );
  return $form;
}

function content_hub_instance_value($key, $entity, $default = '') {
  return is_object($entity) ? $entity->{$key} : $default;
}

/**
 * Admin form for managing/adding content hub instances.
 */
 function content_hub_instances_form($form, $form_state, $entity = FALSE) {

  $form['uuid'] = array(
    '#type' => 'hidden',
    '#title' => t('UUID'),
    '#default_value' => content_hub_instance_value('uuid', $entity),
  );
  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Instance Name'),
    '#description' => t('The instance name of the Content Hub endpoint, e.g. Production'),
    '#default_value' => content_hub_instance_value('name', $entity),
    '#required' => TRUE,
  );
  $form['hostname'] = array(
    '#type' => 'textfield',
    '#title' => t('Content Hub Connector Hostname'),
    '#description' => t('The hostname of the Content Hub connector api, e.g. http://localhost:5000'),
    '#default_value' => content_hub_instance_value('hostname', $entity),
    '#required' => TRUE,
  );

  $form['api_key'] = array(
    '#type' => 'textfield',
    '#title' => t('API Key'),
    '#default_value' => content_hub_instance_value('api_key', $entity),
    '#required' => TRUE,
  );

  $form['secret_key'] = array(
    '#type' => 'password',
    '#title' => t('Secret Key'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save Configuration'),
  );

   return $form;
 }

 /**
 * Implements hook_form_validate().
 */
function content_hub_instances_form_validate($form, &$form_state) {
  $entity = new stdClass;

  if (!empty($form_state['values']['uuid'])) {
    $entity = content_hub_instance_load($form_state['values']['uuid']);
  }

  // Collecting data.
  $entity->hostname = $form_state['values']['hostname'];
  $entity->api_key = $form_state['values']['api_key'];
  $entity->name = $form_state['values']['name'];

  if (!empty($form_state['values']['secret_key'])) {
    $entity->secret_key = $form_state['values']['secret_key'];
  }
  elseif (!empty($entity->secret_key)) {
    $form_state['values']['secret_key'] = $entity->secret_key;
  }


  // Only reset the secret if it is passed.
  $decrypted_secret = $entity->secret_key;
  // If the secret key was empty then try to load the secret key from the
  // drupal variable stored in database.
  // if (empty($decrypted_secret)) {
  //   $decrypted_secret = $entity->secret_key;
  //   // TODO: copy and pasted from content hub connector. Looks broken so commented out.
  //   // If encryption is activated, then encrypt it too.
  //   // if (!empty($encryption)) {
  //   //   $decrypted_secret = content_hub_connector_cipher()->decrypt($decrypted_secret);
  //   // }
  // }

  $client = new ContentHubClient\ContentHub($entity->api_key, $decrypted_secret, '', ['base_url' => $entity->hostname]);

  try {
    $site = $client->ping();
  }
  catch (ConnectException $ex) {
    $msg = $ex->getMessage();
    form_set_error('hostname', t('Could not reach the Content Hub. Please verify your hostname URL. [Error message: @msg]', array(
      '@msg' => $msg,
    )));
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

/**
 * Implements hook_form_submit().
 */
function content_hub_instances_form_submit($form, &$form_state) {
  $entity = (object) $form_state['values'];

  if (empty($entity->uuid)) {
    $entity->is_new = TRUE;
  }
  content_hub_instance_save($entity);

  // $client = clone $entity;
  // $client->instance = $entity->uuid;
  // content_hub_instances_register_client($client);
  // content_hub_instances_client_save($client);
  $form_state['redirect'] = array('admin/config/services/content-hub');
}
